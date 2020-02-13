#pragma once

#include <functional>

namespace utils {
    bool backoff(std::function<bool()> const& callback, std::function<bool()> const& give_up = [] {return false;});

    namespace templates {
        /* Returns the first non-void type from the provided types */
        template <typename... Rest> struct non_void;
        template <> struct non_void<> {using type = void;};
        template <typename T, typename... Rest> struct non_void<T, Rest...> {
            using type = T;
        };
        template <typename... Rest> struct non_void<void, Rest...> {
            using type = typename non_void<Rest...>::type;
        };


        /* Gets the type of the first argument to a callable, returning void if
           there is more than one argument or if it's not callable */
        template <typename T> struct get_arg {
        private:
            // Class member functions
            template <typename U> struct get_class_member_arg {using type = void;};
            template <typename Ret, typename Class, typename Arg>
            struct get_class_member_arg<Ret(Class::*)(Arg) const> {using type = Arg;};
            // Lambdas (or any other class with operator() overloaded)
            template <typename U, typename = void> struct get_callable_arg {using type = void;};
            template <typename U> struct get_callable_arg<U, std::void_t<decltype(&U::operator())>> {
                using type = typename get_class_member_arg<decltype(&U::operator())>::type;
            };
            // Regular function pointers
            template <typename U> struct get_fptr_arg {using type = void;};
            template <typename Ret, typename Arg> struct get_fptr_arg<Ret(*)(Arg)> {using type = Arg;};

        public:
            using type = typename non_void<
                typename get_class_member_arg<T>::type,
                typename get_callable_arg<T>::type,
                typename get_fptr_arg<T>::type
            >::type;
        };

        /* Gets the return type of a callable, returning void is the type is not a callable */
        template <typename T> struct get_ret_type {
        private:
            // Class member functions
            template <typename U> struct get_class_member_ret_type {using type = void;};
            template <typename Ret, typename Class, typename... Args>
            struct get_class_member_ret_type<Ret(Class::*)(Args...) const> {using type = Ret;};
            // Lambdas (or any other class with operator() overloaded)
            template <typename U, typename = void> struct get_callable_ret_type {using type = void;};
            template <typename U> struct get_callable_ret_type<U, std::void_t<decltype(&U::operator())>> {
                using type = typename get_class_member_ret_type<decltype(&U::operator())>::type;
            };
            // Regular function pointers
            template <typename U> struct get_fptr_ret_type {using type = void;};
            template <typename Ret, typename... Args> struct get_fptr_ret_type<Ret(*)(Args...)> {using type = Ret;};

        public:
            using type = typename non_void<
                typename get_class_member_ret_type<T>::type,
                typename get_callable_ret_type<T>::type,
                typename get_fptr_ret_type<T>::type
            >::type;
        };
    }
}
