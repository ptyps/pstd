#pragma once

#include <boost/algorithm/string/join.hpp>
#include <functional>
#include <optional>
#include <cxxabi.h>
#include <variant>
#include <string>
#include <memory>

namespace pstd {
  using vstring = std::string_view;
  using cstring = const char*;

  template <typename ...A>
    std::string format(vstring text, A ...args) {
      auto length = std::snprintf(nullptr, 0, &text[0], args...);
      auto out = std::string(length, '\0');
      std::sprintf(&out[0], &text[0], args...);
      return out.data();
    }

  struct exception : public std::exception {
    private:
      std::string message;

    public:
      template <typename ...A>
        exception(pstd::vstring str, A ...args) {
          message = format(str, args ...);
        }

      pstd::cstring what() const noexcept override {
        return message.data();
      }
  };

  template <typename ...A>
    void log(vstring text, A ...args) {
      auto out = format(text, args ...);

      printf(&out[0]);

      if (!out.ends_with("\n"))
        printf("\n");
    }

  template <typename T, typename ...A>
    bool has(std::variant<A ...> vari) {
      return std::holds_alternative<T>(vari);
    }

  void split(std::string text, vstring d, std::function<void(std::string)> func) {
    for (auto i = text.find(d); i != EOF; i = text.find(d)) {
      func(text.substr(0, i));
      text.erase(0, i + d.size());
    }

    if (!text.empty())
      func(text);
  }

  void rtrim(std::string &str) {
    if (str.empty())
      return;

    auto rbegin = std::rbegin(str);
    auto rend = std::rend(str);

    auto iter = std::find_if(rbegin, rend, [](char ch) {
      return !std::isspace(ch);
    });

   str.erase(iter.base(), str.end());
  }

  template <typename C>
    std::string join(C cont, vstring delm) {
      return boost::algorithm::join(cont, delm);
    }

  bool contains(vstring text, vstring needle) {
    return text.find(needle) != EOF;
  }

  void replace(std::string &text, vstring from, vstring to) {
    auto i = text.find(from);

    if (i == EOF)
      return;

    text.replace(i, from.length(), to);
  }

  // ----

   template <typename T>
    std::string type_name() {
      using type = typename std::remove_reference<T>::type;
      
      auto own = std::unique_ptr<char, void(*)(void*)>({
        abi::__cxa_demangle(typeid(type).name(), nullptr, nullptr, nullptr),
        std::free
      });

      std::string out = own != nullptr ?
        own.get() : typeid(type).name();

      if (std::is_const<type>::value)
          out += " const";

      if (std::is_volatile<type>::value)
          out += " volatile";

      if (std::is_lvalue_reference<T>::value)
          out += "&";

      else if (std::is_rvalue_reference<T>::value)
          out += "&&";

      replace(out, "> >", ">>");
          
      return out;
    }

  template <typename T>
    std::string type_name(T to) {
      return type_name<T>();
    } 

  // ----

  template <typename T>
    struct iter_details : public iter_details<decltype(&T::operator())> {

    };
    
  template <template <typename ...> typename C, typename ...A>
    struct iter_details<C<A...>> {
      using type = typename std::iterator_traits<typename C<A...>::iterator>::value_type;

      template <uint i>
        struct arg {
          typedef typename std::tuple_element<i, std::tuple<A ...>>::type type;
        };
    };

  template <typename T>
    struct func_details : public func_details<decltype(&T::operator())> {

    };
    
  template <typename R, typename C, typename ...A>
    struct func_details<R(C::*)(A...) const> {
      enum { arity = sizeof...(A) };

      using type = std::function<R(A...)>;
      using returns = R;

      template <uint i>
        struct arg {
          typedef typename std::tuple_element<i, std::tuple<A ...>>::type type;
        };
    };

  // ----

  template <template <typename ...> typename C, typename ...A>
    void each(C<A...> cont, std::function<void(typename iter_details<C<A ...>>::type)> func) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter)
        func(*iter);
    }

  template <template <typename ...> typename C, typename ...A>
    void each(C<A...> cont, std::function<void(typename iter_details<C<A ...>>::type, int)> func) {
      auto begin = std::begin(cont);

      for (auto iter = begin; iter != cont.end(); ++iter) {
        auto i = std::distance(begin, iter);
        func(*iter, i);
      }
    }

  template <template <typename ...> typename C, typename ...A>
    bool until(C<A...> cont, std::function<bool(typename iter_details<C<A ...>>::type)> func) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        auto i = func(*iter);

        if (i)
          return !0;
      }

      return !1;
    }

  template <template <typename ...> typename C, typename ...A>
    void remove(C<A...> &cont, typename iter_details<C<A...>>::type find) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        if (*iter != find)
          continue;

        iter = --cont.erase(iter);
      }
    }

  template <template <typename ...> typename C, typename ...A>
    void remove(C<A...> &cont, std::function<bool(typename iter_details<C<A...>>::type)> func) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        if (!func(*iter))
          continue;

        iter = --cont.erase(iter);
      }
    }

  template <template <typename ...> typename C, typename ...A, typename R = typename iter_details<C<A ...>>::type>
    std::optional<R> find(C<A...> &cont, std::function<bool(typename iter_details<C<A...>>::type)> func) {
      for (auto iter = cont.begin(); iter != cont.end(); ++iter) {
        if (!func(*iter))
          continue;

        return *iter;
      }

      return {};
    }

  template <template <typename ...> typename C, typename ...A, typename R = typename iter_details<C<A ...>>::type>
    std::optional<R> pop(C<A...> &cont) {
      if (cont.size() == 0)
        return {};

      auto out = cont.front();
      cont.pop_front();
      
      return out;
    }

  // ----

  template <typename X, typename Y>
    inline constexpr bool is_of_type = std::is_same_v<X, Y>;

  template <typename X>
    inline constexpr bool is_of_void = std::is_void_v<X>;

  template <typename B, typename X>
    inline bool is_of_instance(const X*) {
      return std::is_base_of_v<B, X>;
    }

  template <bool X>
    using enable_if = std::enable_if_t<X, bool>;
}
