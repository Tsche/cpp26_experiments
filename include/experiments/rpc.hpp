#pragma once
#include <experimental/meta>
#include <vector>
#include <ranges>
#include <algorithm>
#include <array>
#include <type_traits>

#include "util/meta.hpp"

namespace rpc {
template <auto, int>
struct RemoteCall;

template <typename T, template <std::meta::info, std::size_t> class Proxy>
consteval auto make_proxy_t(){
    struct RemoteImpl;
    return define_aggregate(^RemoteImpl,
        meta::member_functions_of(^T)
            | std::views::transform([index=0](std::meta::info r) mutable {
                auto type = substitute(^Proxy, {reflect_value(r), std::meta::reflect_value(index++)});
                return data_member_spec(type, {.name=identifier_of(r)});
            }));
}

template <typename T, template <std::meta::info, std::size_t> class Proxy = RemoteCall>
using Remote = [:make_proxy_t<std::remove_cvref_t<T>, Proxy>():];

struct Message {
    enum Kind : std::uint8_t {
        FNC_REQ,
        FNC_RET
    };

    Kind kind;
    std::uint8_t opcode;
    std::uint16_t checksum;
    std::uint32_t size;

    std::array<unsigned char, 56> data{};
};
static_assert(sizeof(Message) == 64);


struct MessageParser{
    explicit MessageParser(Message& message) : message(&message){}
    // TODO replace with proper deserialization machinery
    template <typename T>
    T read_field(){
        T value;
        memcpy(&value, message->data.begin() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }
private:
    Message* message;
    int offset = 0;
};

struct MessageBuilder {
    template <typename T>
    MessageBuilder& add_field(T obj){
        auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(obj);
        std::copy(bytes.begin(), bytes.end(), message.data.begin() + offset);
        offset += bytes.size();
        return *this;
    }

    Message finalize(){
        message.size = offset;
        return message;
    }

    explicit MessageBuilder(Message msg) : message(msg), offset(msg.size) {}
private:
    int offset = 0;
    Message message;
};

template <typename T>
struct Function;
template <typename T, typename... Args>
struct Function<T(Args...)>{
    using return_type = T;
    template <typename C>
    using as_pmf = T (C::*)(Args...);

    template <std::size_t Idx>
    static constexpr Message serialize(Args... args){
        auto builder = MessageBuilder{{.kind=Message::FNC_REQ, .opcode=Idx}};
        (builder.add_field(args), ...);
        return builder.finalize();
    }

    template <auto Fptr, typename Obj>
    static constexpr T eval(Obj&& obj, Message msg){
        auto parser = MessageParser{msg};
        return (obj.*Fptr)(parser.read_field<Args>()...);
    }
};

template <std::meta::info Meta, int Idx>
struct RemoteCall<Meta, Idx>{
    static constexpr auto index = Idx;

    using parent_type = [:parent_of(Meta):];
    using function_type = Function<[:type_of(Meta):]>;
    using member_type = function_type::template as_pmf<parent_type>;
    using return_type = function_type::return_type;
    
    template <typename Sink>
    return_type operator()(Sink& client, auto... args) const {
        auto msg = function_type::template serialize<Idx>(args...);
        client.send(msg);

        // block until recv returns
        if constexpr (std::is_void_v<return_type>){
            client.recv();
        } else {
            auto msg = client.recv();
            auto parser = MessageParser{msg};
            return parser.read_field<return_type>();
        }
    }

    template <typename Obj>
    static Message eval(Obj obj, Message msg) {
        static constexpr member_type fnc = extract<member_type>(Meta);
        auto ret = Message{.kind=Message::FNC_RET, .opcode=msg.opcode};

        if constexpr (std::is_void_v<return_type>){
            function_type::template eval<fnc>(obj, msg);
        } else {
            auto retval = function_type::template eval<fnc>(obj, msg);
            ret = MessageBuilder{ret}.add_field(retval).finalize(); 
        }
        return ret;
    }
};


namespace impl{
template <typename T, std::meta::info... Members>
constexpr Message do_dispatch(T&& obj, Message msg){
    constexpr static auto proxy = Remote<T>{};
    using dispatch_type = Message(*)(std::remove_cvref_t<T>, Message);
    dispatch_type fnc = nullptr;
    (void)((proxy.[:Members:].index == msg.opcode ? fnc = &decltype(proxy.[:Members:])::eval, true : false) || ...);
    return fnc(obj, msg);
}

template <typename T>
consteval auto get_dispatcher(){
    using proxy_type = Remote<T>;

    std::vector args = {^T};
    for (auto member: nonstatic_data_members_of(^proxy_type)){
        args.push_back(reflect_value(member));
    }
    return std::meta::extract<Message(*)(T&&, Message)>(substitute(^do_dispatch, args));
}
}

struct Service {
template <typename T>
auto dispatch(this T&& self, Message msg){
    constexpr static auto dispatcher = impl::get_dispatcher<T>();
    return dispatcher(self, msg);
}
};

}