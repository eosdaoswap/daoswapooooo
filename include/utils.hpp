#include <types.hpp>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/action.hpp>
#include <eosio/contract.hpp>
#include <eosio/crypto.hpp>
#include <eosio/ignore.hpp>
#include <math.h>
#include <ctime>
#include <string>

namespace utils
{
    using std::string;

    void inline_transfer(const name& contract,const name& from,const name& to,const asset& quantity,const string& memo)
    {
        action(
            permission_level{from, "active"_n},
            contract,
            name("transfer"),
            make_tuple(from, to, quantity, memo))
            .send();
    }   

    void transfer( const name& from, const name& to, const extended_asset& value, const string& memo ) 
    {
        inline_transfer(value.contract,from,to,value.quantity,memo);
    }

    static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
    {
        stats statstable( token_contract_account, sym_code.raw() );
        const auto& st = statstable.get( sym_code.raw() );
        return st.supply;
    }

    static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
    {
        accounts accountstable( token_contract_account, owner.value );
        const auto& ac = accountstable.get( sym_code.raw() );
        return ac.balance;
    }    
    
    uint64_t toInt(const std::string& str) 
    {
        if (str.empty() || str == "") {
            return 0;
        }
        else {
            std::string::size_type sz = 0;
            return std::stoull(str, &sz, 0);
        }
    }     

    std::vector<string> split(const string &str, const string &delim)
    {
      std::vector<string> strs;
      size_t prev = 0, pos = 0;
      do
      {
         pos = str.find(delim, prev);
         if (pos == string::npos)
            pos = str.length();
         string s = str.substr(prev, pos - prev);
         if (!str.empty())
            strs.push_back(s);
         prev = pos + delim.length();
      } while (pos < str.length() && prev < str.length());
      return strs;
    }

    std::vector<string> split2(const string &str, const uint64_t &len)
    {
      std::vector<string> strs;
      size_t prev = 0, pos = 0;
      do
      {
         string s = str.substr(prev,len);
         if (!str.empty())
            strs.push_back(s);
         pos++;   
         prev =  pos * len;
      } while ((prev + len) <= str.length());
      return strs;
    }


    static symbol_code get_lptoken_from_pairid(uint64_t pairid)
    {
        if(pairid == 0) return {};
        std::string res;
        while(pairid)
        { 
            uint64_t letter =  pairid % 26;
            letter = letter == 0 ? 0: (letter -1);
            res = (char)('A' + letter) + res;
            pairid /= 26;
        }
        return eosio::symbol_code{ "DAO" + res };
    }

    static uint64_t get_pairid_from_lptoken(symbol_code lp_symcode)
    {
        std::string str = lp_symcode.to_string();
        if(str.length() < 3) return 0;
        uint64_t res = 0;
        if(str[0]!='D' || str[1]!='A' || str[2]!='O') return 0;
        for(auto i = 3; i < str.length(); i++){
                res *= 26;
                res += str[i] - 'A' + 1;
        }
        return res;
    }

    static name parse_name(const string& str) {

        if(str.length()==0 || str.length()>13) return {};
        int i=-1;
        for (const auto c: str ) {
            i++;
            if( islower(c) || (isdigit(c) && c<='6') || c=='.') {
                if(i==0 && !islower(c) ) return {};
                if(i==12 && (c<'a' || c>'j')) return {};
            }
            else return {};
        }
        return name{str};
    } 

    static symbol_code parse_symbol_code(const string& str) {
        if(str.size() > 7) return {};

        for (const auto c: str ) {
            if( c < 'A' || c > 'Z') return {};
        }
        const symbol_code sym_code = symbol_code{ str };

        return sym_code.is_valid() ? sym_code : symbol_code{};
    }   

    bool is_digit( const string str )
    {
        if ( !str.size() ) return false;
        for ( const auto c: str ) {
            if ( !isdigit(c) ) return false;
        }
        return true;
    }

    static uint64_t add( const uint64_t x, const uint64_t y ) {
        const uint64_t z = x + y;
        check( z >= x, "600407:safemath-add-overflow"); return z;
    }

    static uint64_t sub(const uint64_t x, const uint64_t y) {
        const uint64_t z = x - y;
        check(z <= x, "600407:safemath-sub-overflow"); return z;
    }    


    static uint128_t mul(const uint64_t x, const uint64_t y) {
        const uint128_t z = static_cast<uint128_t>(x) * y;
        check(y == 0 || z / y == x, "600407:safemath-mul-overflow"); return z;
    }

    static uint128_t mul128(const uint128_t x, const uint128_t y) {
        const uint128_t z = x * y;
        check(y == 0 || z / y == x, "600407:safemath-mul-overflow"); return z;
    }


    static uint64_t div(const uint64_t x, const uint64_t y) {
        check(y > 0, "600407:safemath-divide-zero");
        return x / y;
    }

    static uint64_t div128(const uint128_t x, const uint128_t y) {
        check(y > 0, "600407:safemath-divide-zero");
        return x / y;
    }    

    static int64_t mul_amount( const int64_t amount, const uint8_t precision0, const uint8_t precision1 )
    {
        const int64_t res = static_cast<int64_t>( precision0 >= precision1 ? mul(amount, pow(10, precision0 - precision1 )) : amount / static_cast<int64_t>(pow( 10, precision1 - precision0 )));
        check(res >= 0, "600407:mul_amount: mul/div overflow"+std::to_string(amount)+"|"+std::to_string(precision0)+"|"+std::to_string(precision1)+"|"+std::to_string(res));
        return res;
    } 

    static int128_t mul_amount128( const int128_t amount, const uint8_t precision0, const uint8_t precision1 )
    {
        const int128_t res = static_cast<int128_t>( precision0 >= precision1 ? mul128(amount, pow(10, precision0 - precision1 )) : amount / static_cast<int128_t>(pow( 10, precision1 - precision0 )));
        check(res >= 0, "600407:mul_amount128: mul/div overflow ");
        return res;
    }    

    static int64_t div_amount( const int64_t amount, const uint8_t precision0, const uint8_t precision1 )
    {
        return precision0 >= precision1 ? amount / static_cast<int64_t>(pow( 10, precision0 - precision1 )) : mul(amount, pow( 10, precision1 - precision0 ));
    }  


    static uint64_t issue( const uint64_t payment, const uint64_t deposit, const uint64_t supply, const uint16_t ratio = 10000 )
    {
        check(payment > 0, "600408:Amount must be greater than zero");
        if ( supply == 0 ) return payment * ratio;
        return ((uint128_t(deposit + payment) * supply) / deposit) - supply;
    }    

    static uint128_t retire( const uint128_t payment, const uint128_t deposit, const uint128_t supply )
    {
        check(payment > 0, "600408:Amount must be greater than zero");
        check(deposit > 0, "600408:Amount must be greater than zero");
        check(supply > 0, "600408:Amount must be greater than zero");

        return (payment * deposit) / supply;
    }

    //static uint64_t get_amount_out( const uint64_t amount_in, const uint64_t reserve_in, const uint64_t reserve_out, const uint16_t fee = 30 )
    //{
    //    // checks
    //    check(amount_in > 0, "600408:Amount must be greater than zero");
    //    check(reserve_in > 0 && reserve_out > 0, "600408:Amount must be greater than zero");

        // calculations
    //    const uint128_t amount_in_with_fee = static_cast<uint128_t>(amount_in) * (10000 - fee);
    //    const uint128_t numerator = amount_in_with_fee * reserve_out;
    //    const uint128_t denominator = (static_cast<uint128_t>(reserve_in) * 10000) + amount_in_with_fee;
    //    const uint64_t amount_out = numerator / denominator;

    //    return amount_out;
    //}    

    
    static uint64_t get_amount_out( const uint128_t amount_in, const uint128_t reserve_in, const uint128_t reserve_out,const uint8_t precision0, const uint8_t precision1, const uint16_t fee = 30)
    {
        // checks
        check(amount_in > 0, "600408:Amount must be greater than zero");
        check(reserve_in > 0 && reserve_out > 0, "600408:Amount must be greater than zero");

        // calculations
        const uint128_t amount_in_with_fee = static_cast<uint128_t>(amount_in) * (10000 - fee);
        const uint128_t numerator = amount_in_with_fee * reserve_out;
        const uint128_t denominator = (static_cast<uint128_t>(reserve_in) * 10000) + amount_in_with_fee;
        const uint128_t amount = numerator / denominator;

        const uint64_t amount_out = static_cast<uint64_t>(precision0 >= precision1 ? amount / static_cast<int128_t>(pow( 10, precision0 - precision1 )) : mul128(amount, pow( 10, precision1 - precision0 )));
        return amount_out;
    } 

    static uint64_t get_amount_out2(const uint64_t out, const uint128_t amount_in_c, const uint128_t reserve_in_c, const uint128_t reserve_out_c)
    {
        const uint64_t amount_out =static_cast<uint64_t>(out * reserve_out_c * amount_in_c / reserve_in_c);
        return amount_out;
    }

    string to_hex(const char *d, uint32_t s)
    {
        std::string r;
        const char *to_hex = "0123456789abcdef";
        uint8_t *c = (uint8_t *)d;
        for (uint32_t i = 0; i < s; ++i)
            (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
        return r;
    }       

    string sha256_to_hex(const checksum256 &sha256)
    {
        auto hash_data = sha256.extract_as_byte_array();
        return to_hex((char *)hash_data.data(), sizeof(hash_data.data()));
    }

    std::string get_md5(std::string verify)
    {
        const char *verify_cstr = verify.c_str();
        checksum256 digest = sha256(verify_cstr, strlen(verify_cstr));
        return sha256_to_hex(digest);
    }

    uint64_t uint64_hash(const string &hash)
    {
        return std::hash<string>{}(hash);
    }    

}
