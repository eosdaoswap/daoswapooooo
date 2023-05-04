#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <utils.hpp>
#define max(a,b) (a>b?a:b)

using namespace eosio;

class [[eosio::contract("daoswap")]] daoswap : public eosio::contract
{   

public:

    using contract::contract;

    [[eosio::on_notify("*::transfer")]]
    void on_transfer( const name& from, const name& to, const asset& quantity, const std::string& memo  );

    [[eosio::action]]
    void deposit( const name& owner, const symbol_code& pair_id, const std::optional<int64_t> min_amount );

    [[eosio::action]]
    void cancel( const name& owner, const symbol_code& pair_id );

    [[eosio::action]]
    void withdraw( const name& owner, const asset& quantity );

    [[eosio::action]]
    void createpair( const name& creator,const name& contract0, const symbol_code& code0,const name& contract1, const symbol_code& code1);

    [[eosio::action]]
    void removepair(const name& mgr, const symbol_code& pair_id );

    [[eosio::action]]
    void lockliq(const name& owner,const asset& quantity,const uint64_t& exptime);

    [[eosio::action]]
    void unlockliq(const name& owner,const asset& quantity);



    [[eosio::action]]
    void init(const name& mgr_account,const symbol_code& pair_id );

    [[eosio::action]]
    void reset();
    
    [[eosio::action]]
    void setfee(const name& mgr,const uint8_t& trade_fee, const std::optional<uint8_t> protocol_fee, const std::optional<name> fee_account );

    [[eosio::action]]
    void setnotifiers(const name& mgr, const std::vector<name> notifiers );

    [[eosio::action]]
    void setstatus(const name& mgr, const name& status );

    [[eosio::action]]
    void setmintimes(const name& mgr,const uint64_t& minlocktimes);

    [[eosio::action]]
    void liquiditylog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& liquidity, const extended_asset& liquidity0,  const extended_asset& liquidity1, const asset& total_liquidity, const asset& reserve0, const asset& reserve1);
    
    [[eosio::action]]
    void swaplog( const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& quantity_in, const extended_asset& quantity_out, const asset& protocol_fee,const asset& trade_fee, const double& trade_price, const asset& reserve0, const asset& reserve1  );

    [[eosio::action]]
    void tokenlog(const symbol_code& pair_id, const name& owner,const name& action,const extended_asset& liquidity,const uint64_t& exptime);

    [[eosio::action]]
    void pairidlog(const symbol_code& pair_id,const name& owner,const name& action);

    [[eosio::action]]
    void pledgeslog(const symbol_code& pair_id, const name& owner,const name& action,const uint64_t& pledgeid,const extended_asset& liquidity,const uint64_t& receivetime,const uint64_t& expiredtime,const uint64_t& cdsid);

    [[eosio::action]]  
    void pledgeparam(const name& mgr,const uint64_t& pledgeid,const name& contract, const symbol_code& sym,const uint64_t& annualized,const uint64_t& duration);

    [[eosio::action]]  
    void pledgecond(const name& mgr,const uint64_t& pledgeid,const asset& min_amount,const asset& max_amount, const uint8_t& oracle,const uint64_t& oracleParam);

    [[eosio::action]]  
    void pledgestatus(const name& mgr,const uint64_t& pledgeid, const uint8_t& status);

    [[eosio::action]]
    void unpledge(const name& owner,const uint64_t& cdsid);
   
    [[eosio::action]]   
    void sendlog(const name& from, const extended_asset& ext_quantity,const std::string& memo);

    [[eosio::action]]   
    void receivelog(const name& owner,const name& to,const name& contract, const asset& quantity);

    [[eosio::action]]  
    void receive(const name& owner,const name& to,const name& contract, const asset& quantity);
   

private:

    struct [[eosio::table("config")]] config_row {
        name                status = "testing"_n;
        uint8_t             trade_fee = 20;
        uint8_t             protocol_fee = 10;
        name                fee_account;
        name                mgr_account;     
        symbol_code         pair_id;               
        std::vector<name>   notifiers;
        uint64_t            minlocktimes;
    };
    typedef eosio::singleton<name("config"), config_row > config_table;


    struct [[eosio::table("orders")]] orders_row {
        name                owner;
        extended_asset      quantity0;
        extended_asset      quantity1;

        uint64_t primary_key() const { return owner.value; }
    };
    typedef eosio::multi_index<name("orders"), orders_row> orders_table;


    struct [[eosio::table("pairs")]] pairs_row {
        symbol_code         id;
        extended_asset      reserve0;
        extended_asset      reserve1;
        extended_asset      liquidity;
        extended_asset      lock_liq;       
        uint64_t            amplifier;
        double              virtual_price;
        double              price0_last;
        double              price1_last;
        asset               volume0;
        asset               volume1;
        uint64_t            trades;
        time_point_sec      last_updated;

        uint64_t primary_key() const { return id.raw(); }
    };
    typedef eosio::multi_index<name("pairs"), pairs_row> pairs_table;


    struct [[eosio::table("markets")]] markets_row {
        name owner;
        asset liquidity;
        asset lock_liq;
        uint64_t primary_key() const { return owner.value;  }
    };
    typedef multi_index<name("markets"), markets_row> markets_table;   


    struct [[eosio::table("lockdtl")]] lockdtl_row {
        uint64_t            id;    
        symbol_code         pair_id;
        asset               quantity;
        uint64_t            acttime;
        uint64_t            exptime;

        uint64_t primary_key() const { return id; }
        uint64_t by_pairid() const { return pair_id.raw(); }     
    };
    typedef multi_index<name("lockdtl"), lockdtl_row,    
         indexed_by<name("pairid"), const_mem_fun<lockdtl_row, uint64_t, &lockdtl_row::by_pairid>>                       
      > lockdtl_table;


    struct [[eosio::table("pledgepara")]] pledgepara_row 
    {
            uint64_t        pledgeid;
            name            contract; 
            symbol_code     sym;         
            uint64_t        annualized;
            uint64_t        duration;    
            asset           min_amount;
            asset           max_amount;  
            uint8_t         oracle; 
            uint64_t        oracleParam;                                              
            uint8_t         status;                
            
            uint64_t primary_key() const { return pledgeid; }     
    };
    typedef eosio::multi_index<name("pledgepara"), pledgepara_row> pledgepara_table;    


    struct [[eosio::table("pledgelist")]] pledgelist_row {
        uint64_t            cdsid;
        uint64_t            pledgeid;        
        extended_asset      liquidity;
        uint64_t            receive_time;
        uint64_t            expired_time;       

        uint64_t primary_key() const { return cdsid; }
        uint64_t by_pledgeid() const { return pledgeid; }       
        uint64_t by_receive() const { return receive_time; }
        uint64_t by_expired() const { return expired_time; }      
    };
    typedef multi_index<name("pledgelist"), pledgelist_row,  
         indexed_by<name("pledgeid"), const_mem_fun<pledgelist_row, uint64_t, &pledgelist_row::by_pledgeid>>,       
         indexed_by<name("receivetime"), const_mem_fun<pledgelist_row, uint64_t, &pledgelist_row::by_receive>>,  
         indexed_by<name("expiredtime"), const_mem_fun<pledgelist_row, uint64_t, &pledgelist_row::by_expired>>                                      
      > pledgelist_table;


    struct [[eosio::table("bank")]] bank_row {
        asset      balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("bank"), bank_row> bank_table;


    struct [[eosio::table("swap")]] swap_row {
        asset      balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("swap"), swap_row> swap_table;


    struct [[eosio::table("mixup")]] mixup_row {
        asset      balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("mixup"), mixup_row> mixup_table;


    void _convert( const name& owner, const extended_asset& ext_in, const std::vector<symbol_code> pair_ids, const int64_t& min_return );
    extended_asset _apply_trade( const name& owner, const extended_asset& ext_quantity, const std::vector<symbol_code> pair_ids );
    asset _get_amount_out( const asset& in, const symbol_code& pair_id);
    uint64_t _get_amplifier( const symbol_code& pair_id);
    double _calculate_price( const asset& value0, const asset& value1 );
    double _calculate_virtual_price( const asset& value0, const asset& value1, const asset& supply );

    void _add_deposit( const name& owner, const symbol_code& pair_id, const extended_asset& value );
    void _add_liquidity(const symbol_code& pair_id, const name& owner, const asset& liquidity, const asset& quantity0,  const asset& quantity1);
    void _sub_liquidity(const symbol_code& pair_id, const name& owner, const asset& liquidity, const asset& quantity0,  const asset& quantity1);

    void _verify_mgr( const name& mgr);
    memo_schema _parse_memo( const std::string& memo );
    std::vector<symbol_code> _parse_memo_pair_ids( const std::string& memo );
    void _notify();

    void _liquiditylog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& liquidity, const extended_asset& liquidity0,  const extended_asset& liquidity1, const asset& total_liquidity, const asset& reserve0, const asset& reserve1);
    void _swaplog(const symbol_code& pair_id, const name& owner, const name& action, const extended_asset& quantity_in, const extended_asset& quantity_out, const asset& protocol_fee,const asset& trade_fee, const double& trade_price, const asset& reserve0, const asset& reserve1 );
    void _tokenlog(const symbol_code& pair_id, const name& owner,const name& action,const extended_asset& liquidity,const uint64_t& exptime);
    void _pairidlog(const symbol_code& pair_id,const name& owner,const name& action);
    void _pledgeslog(const symbol_code& pair_id, const name& owner,const name& action,const uint64_t& pledgeid,const extended_asset& liquidity,const uint64_t& receivetime,const uint64_t& expiredtime,const uint64_t& cdsid);

    void _add_pledge(const name& owner,const extended_asset& ext_quantity,const uint64_t& pledgeid);

    void _add_bank(const name& scope,const asset& value);
    void _sub_bank(const name& scope,const asset& value);

    void _add_swap(const name& scope,const asset& value);
    void _sub_swap(const name& scope,const asset& value);


    void _sendlog(const name& from, const extended_asset& ext_quantity,const std::string& memo);
    void _receivelog(const name& owner,const name& to,const name& contract, const asset& quantity);
    void _add_send( const name& from, const extended_asset& ext_quantity,const std::string& memo); 
    void _add_mixup(const uint64_t& scope,const asset& value);
    void _sub_mixup(const uint64_t& scope,const asset& value );

};
