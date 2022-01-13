#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

// inspired by https://github.com/kennytm/utils/blob/master/traits.hpp
// and https://stackoverflow.com/a/28213747. Further thanks to G. Sliepen
// on StackExchange CodeReview for numerous helpful suggestions.
// 
// invocable_traits inspects a callable and allows to query information about
// its return type, host class (if member function or data member, this
// includes lambdas), arity, argument types, and several properties of the
// function declaration. Specifically, the following properties are exposed:
// arity            : std::size_t value indicating the arity of the callable
//                    (not counting variadic input arguments, so int(int,...)
//                    has arity 1)
// is_const         : true if the callable is declared const
// is_volatile      : true if the callable is declared volatile
// is_noexcept      : true if the callable is declared noexcept
// is_variadic      : true if the callable has an old-/C-style variadic
//                    argument
// declared_result_t: return type as declared in the callable signature
// invoke_result_t  : return type when the callable is std::invoke()d
// class_t          : class this callable is a member of, void if free
//                    function
// arg_t            : indexable list of the declared argument types of
//                    the callable (e.g. use arg_t<0> to retrieve the
//                    first argument type)
//
// When the user has provided argument types and they are used (see below),
// three additional properties are exposed:
// num_matched_overloads : number of overloads found that matched the
//                         user's provided argument types.
// is_exact_match        : true if the user-provided argument types
//                         were an exact match to an overload, false
//                         if the overload was found through the search
//                         procedure (see 2 below).
// matched_overload      : indexable list of matched overloads, each
//                         containing all the info about the matched
//                         callable, as described above. Use, e.g.
//                         matched_overload<0>::arg_t<0> to query the
//                         declared type of the first argument of the
//                         first found overload.
// 
// Note that this machinery does not handle overloaded or templated functions
// (but see below for the special case of overloaded operator()). It could
// not possibly do so, since these do not have a unique address. If references
// or pointers to specific overloads or template instantiations are passed,
// all works as expected.
// 
// To handle overloaded or templated operator() of functors (this includes
// lambdas), you can help invocable_traits find the right overload by
// providing additional type arguments, e.g., invocable_traits<Functor, int>.
// If required for disambiguation (i.e. if the passed callable is indeed a
// functor with an overloaded or templated operator()), these extra type
// arguments are used to resolve the desired overload. If the additional type
// arguments are not required to resolve the overload, or a callable other
// than a functor is passed, the additional type arguments are ignored. It is
// thus not an error to provide the additional type arguments (e.g. if you
// know what input arguments your callable should take), but the callable
// will not be checked for a matching signature unless the additional type
// arguments were required for overload resolution/template instantiation.
// Use of std::is_invocable<> is therefore adviced in all cases where you
// provide additional type arguments to invocable_traits.
// 
// When resolving the overload using these extra type arguments, two things
// may happen:
// 1. the user provides the exact argument types (including const and
//    reference qualifiers) of an existing overload. In that case,
//    invocable_traits for only that overload are returned.
// 2. the user provides argument types that do not exactly match an
//    existing overload. In that case, invocable_traits generates all
//    possible const and reference qualified versions of the provided
//    argument types, and tests for overloads with all their permutations.
//    If any are found, invocable_traits for the first found overload are
//    returned, and invocable_traits for any additional matching overloads
//    can also be retrieved (see propeties above).
// When both procedures fail to yield a matching overload, a static_assert
// is raised.
// 
// Note that even though std::is_invocable<> may yield true, there are
// various situations where invocable_traits will fail to find the right
// overload when provided with the same type arguments as
// std::is_invocable<>. These include at least:
// - implicit conversions of the argument type
// - old-/C-style variadic functions
// - default arguments
// To be able to find an overload, the correct type (excluding qualifiers,
// see procedure 2 above) of all input arguments (also default arguments)
// must be specified, and variadic inputs should not be specified (the
// presence of these will be deduced by invocable_traits and signalled
// through invocable_traits::is_variadic = true).

namespace invocable_traits
{
enum class Error
{
    None,
    NotAClass,
    NoCallOperator,
    IsOverloadedTemplated,
    OverloadNotResolved,
    Unknown
};

template <Error E>
void issue_error()
{
    static_assert(E != Error::NotAClass,
        "passed type is not a class, and thus cannot have an operator()");
    static_assert(E != Error::NoCallOperator,
        "passed type is a class that doesn't have an operator()");
    static_assert(E != Error::IsOverloadedTemplated,
        "passed passed type is a class that has an overloaded or templated operator(), specify argument types in invocable_traits invocation to disambiguate the operator() signature you wish to use");
    static_assert(E != Error::OverloadNotResolved,
        "passed type is a class that doesn't have an operator() that declares the specified argument types, or some const/ref-qualified combination of the specified argument types");
    static_assert(E != Error::Unknown,
        "an unknown error occurred");

    const char a[] = "test";
};

namespace detail
{
    template <bool, std::size_t i, typename... Args>
    struct invocable_traits_arg_impl
    {
        using type = std::tuple_element_t<i, std::tuple<Args...>>;
    };
    template <std::size_t i, typename... Args>
    struct invocable_traits_arg_impl<false, i, Args...>
    {
        static_assert(i < sizeof...(Args), "Argument index out of bounds (queried callable does not declare this many arguments)");

        // to reduce excessive compiler error output
        using type = void;
    };

    template <
        typename Rd, typename Ri, typename C,
        bool IsConst, bool isVolatile, bool isNoexcept, bool IsVariadic,
        typename... Args>
    struct invocable_traits_class
    {
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr auto is_const    = IsConst;
        static constexpr auto is_volatile = isVolatile;
        static constexpr auto is_noexcept = isNoexcept;
        static constexpr auto is_variadic = IsVariadic;

        using declared_result_t = Rd;   // return type as declared in function
        using invoke_result_t   = Ri;   // return type of std::invoke() expression
        using class_t           = C;

        template <std::size_t i>
        using arg_t = typename invocable_traits_arg_impl<i < sizeof...(Args), i, Args...>::type;

        static constexpr Error error      = Error::None;
    };

    template <
        typename Rd, typename Ri,
        bool IsConst, bool isVolatile, bool isNoexcept, bool IsVariadic,
        typename... Args>
    struct invocable_traits_free : public invocable_traits_class<Rd, Ri, void, IsConst, isVolatile, isNoexcept, IsVariadic, Args...> {};


    // machinery to extract exact function signature and qualifications
    template <typename, typename...>
    struct invocable_traits_impl;

    // pointers to data members
    template <typename C, typename R>
    struct invocable_traits_impl<R C::*>
        : public invocable_traits_class<R,
                                        std::invoke_result_t<R C::*, C>,
                                        C,
                                        false, false, false, false
                                       > {};

    // pointers to functions
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...)>                 : public invocable_traits_impl<R(Args...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...) noexcept>        : public invocable_traits_impl<R(Args...) noexcept> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...)>            : public invocable_traits_impl<R(Args..., ...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...) noexcept>   : public invocable_traits_impl<R(Args..., ...) noexcept> {};

    template <typename...>
    struct typelist {};

#   define IS_NONEMPTY(...) 0 __VA_OPT__(+1)
#   define MAKE_CONST(...)    __VA_OPT__(const)
#   define MAKE_VOLATILE(...) __VA_OPT__(volatile)
#   define MAKE_NOEXCEPT(...) __VA_OPT__(noexcept)
#   define MAKE_VARIADIC(...) __VA_OPT__(, ...)

    // functions, pointers to member functions and machinery to select a specific overloaded operator()
#   define INVOCABLE_TRAITS_SPEC(c,vo,e,va)                                             \
    /* functions */                                                                     \
    template <typename R, typename... Args>                                             \
    struct invocable_traits_impl<R(Args... MAKE_VARIADIC(va))                           \
                                 MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e)>      \
        : public invocable_traits_free<                                                 \
            R,                                                                          \
            std::invoke_result_t<R(Args... MAKE_VARIADIC(va))                           \
                                 MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e), Args...>,    \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(vo),                                                            \
            IS_NONEMPTY(e),                                                             \
            IS_NONEMPTY(va),                                                            \
            Args...> {};                                                                \
    /* pointers to member functions */                                                  \
    template <typename C, typename R, typename... Args>                                 \
    struct invocable_traits_impl<R(C::*)(Args... MAKE_VARIADIC(va))                     \
                                 MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e)>      \
        : public invocable_traits_class<                                                \
            R,                                                                          \
            std::invoke_result_t<R(C::*)(Args...MAKE_VARIADIC(va))                      \
                                 MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e), C, Args...>, \
            C,                                                                          \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(vo),                                                            \
            IS_NONEMPTY(e),                                                             \
            IS_NONEMPTY(va),                                                            \
            Args...> {};                                                                \
    /* machinery to select a specific overloaded operator() */                          \
    template <typename C, typename... OverloadArgs>                                     \
    auto invocable_traits_resolve_overload(std::invoke_result_t<C, OverloadArgs...>         \
                                           (C::*func)(OverloadArgs... MAKE_VARIADIC(va))    \
                                           MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e),\
                                           typelist<OverloadArgs...>)\
    { return func; };

    // cover all const, volatile and noexcept permutations
    INVOCABLE_TRAITS_SPEC( ,  , ,  )
    INVOCABLE_TRAITS_SPEC( ,Va, ,  )
    INVOCABLE_TRAITS_SPEC( ,  ,E,  )
    INVOCABLE_TRAITS_SPEC( ,Va,E,  )
    INVOCABLE_TRAITS_SPEC( ,  , ,Vo)
    INVOCABLE_TRAITS_SPEC( ,Va, ,Vo)
    INVOCABLE_TRAITS_SPEC( ,  ,E,Vo)
    INVOCABLE_TRAITS_SPEC( ,Va,E,Vo)
    INVOCABLE_TRAITS_SPEC(C,  , ,  )
    INVOCABLE_TRAITS_SPEC(C,Va, ,  )
    INVOCABLE_TRAITS_SPEC(C,  ,E,  )
    INVOCABLE_TRAITS_SPEC(C,Va,E,  )
    INVOCABLE_TRAITS_SPEC(C,  , ,Vo)
    INVOCABLE_TRAITS_SPEC(C,Va, ,Vo)
    INVOCABLE_TRAITS_SPEC(C,  ,E,Vo)
    INVOCABLE_TRAITS_SPEC(C,Va,E,Vo)
    // clean up
#   undef INVOCABLE_TRAITS_SPEC
#   undef IS_NONEMPTY
#   undef MAKE_CONST
#   undef MAKE_VOLATILE
#   undef MAKE_NOEXCEPT
#   undef MAKE_VARIADIC

    // check if passed type has an operator(), can be true for struct/class, includes lambdas
    template <typename C>
    concept HasCallOperator = requires(C t)
    {
        t.operator();
    };
    // check if we can get operator().
    // If it fails (and assuming above HasCallOperator does pass),
    // this is because the operator is overloaded or templated
    template <typename T>
    concept CanGetCallOperator = requires
    {
        invocable_traits_impl<
            decltype(
                &T::operator()
            )>();
    };
    // check if we can get an operator() that takes the specified arguments types.
    // If it fails (and assuming above HasCallOperator does pass),
    // an operator() with this specified cv- and ref-qualified argument types does
    // not exist.
    template <typename C, typename... OverloadArgs>
    concept HasSpecificCallOperator = requires
    {
        invocable_traits_resolve_overload<C>(
            &C::operator(),
            typelist<OverloadArgs...>{}
        );
    };
    namespace try_harder
    {
        // cartesian product of type lists, from https://stackoverflow.com/a/59422700/3103767
        template <typename... Ts>
        typelist<typelist<Ts>...> layered(typelist<Ts...>);

        template <typename... Ts, typename... Us>
        auto operator+(typelist<Ts...>, typelist<Us...>)
            ->typelist<Ts..., Us...>;

        template <typename T, typename... Us>
        auto operator*(typelist<T>, typelist<Us...>)
            ->typelist<decltype(T{} + Us{})...>;

        template <typename... Ts, typename TL>
        auto operator^(typelist<Ts...>, TL tl)
            -> decltype(((typelist<Ts>{} *tl) + ...));

        template <typename... TLs>
        using product_t = decltype((layered(TLs{}) ^ ...));

        // adapter to make cartesian product of a list of typelists
        template <typename... Ts>
        auto list_product(typelist<Ts...>)
            ->product_t<Ts...>;

        template <typename T>
        using list_product_t = decltype(list_product(T{}));

        // code to turn input argument type T into all possible const/ref-qualified versions
        template <typename T> struct type_maker_impl;
        // T* -> T*, T* const
        template <typename T>
            requires std::is_pointer_v<T>
        struct type_maker_impl<T>
        {
            using type = typelist<T, const T>;
        };
        // T -> T, T&, const T&, T&&, const T&& (NB: const on const T is ignored, so that type is not included)
        template <typename T>
            requires (!std::is_pointer_v<T>)
        struct type_maker_impl<T>
        {
            using type = typelist<T, T&, const T&, T&&, const T&&>;
        };

        template <typename T>
        struct type_maker : type_maker_impl<std::remove_cvref_t<T>> {};

        template <typename T>
        using type_maker_t = typename type_maker<T>::type;

        template <typename ...Ts>
        struct type_maker_for_typelist
        {
            using type = typelist<type_maker_t<Ts>...>;
        };

        // code to filter out combinations of qualified input arguments that do not
        // resolve to a declared overload
        template <typename, typename> struct concat;
        template <typename T, typename ...Args>
        struct concat<T, typelist<Args...>>
        {
            using type = typelist<T, Args...>;
        };

        template <typename...> struct check;
        template <typename C, typename... Args>
        struct check<C, typelist<Args...>>
        {
            static constexpr bool value = HasSpecificCallOperator<C, Args...>;
        };

        template <typename...> struct filter;
        template <typename C>
        struct filter<C, typelist<>>
        {
            using type = typelist<>;
        };
        template <typename C, typename Head, typename ...Tail>
        struct filter<C, typelist<Head, Tail...>>
        {
            using type = std::conditional_t<check<C, Head>::value,
                typename concat<Head, typename filter<C, typelist<Tail...>>::type>::type,
                typename filter<C, typelist<Tail...>>::type
            >;
        };

        // extract first element from a typelist
        template <typename, typename...> struct get_head;
        template <typename Head, typename ...Tail>
        struct get_head<typelist<Head, Tail...>>
        {
            using type = Head;
        };
    }

    // to reduce excessive compiler error output
    struct invocable_traits_error
    {
        static constexpr std::size_t arity       = 0;
        static constexpr auto        is_const    = false;
        static constexpr auto        is_volatile = false;
        static constexpr auto        is_noexcept = false;
        static constexpr auto        is_variadic = false;
        using declared_result_t                  = void;
        using invoke_result_t                    = void;
        using class_t                            = void;
        template <size_t i>
        using arg_t                              = void;
    };
    struct invocable_traits_error_overload
    {
        static constexpr std::size_t num_matched_overloads = 0;
        static constexpr auto        is_exact_match        = false;
        template <size_t i>
        using matched_overload                             = invocable_traits_error;
    };

    template <typename T, bool hasOverloadArgs>
    constexpr Error get_error()
    {
        if constexpr (!std::is_class_v<T>)
            return Error::NotAClass;
        else if constexpr (!HasCallOperator<T>)
            return Error::NoCallOperator;
        else if constexpr (hasOverloadArgs)
            return Error::OverloadNotResolved;
        else if constexpr (!hasOverloadArgs && !CanGetCallOperator<T>)
            return Error::IsOverloadedTemplated;

        return Error::Unknown;
    }

    template <typename C, typename Head>
    struct get_overload_info
    {
        using type =
            invocable_traits_impl<
                std::decay_t<
                    decltype(
                        invocable_traits_resolve_overload<C>(
                            &C::operator(),
                            Head{}
                        )
                    )
                >
            > ;
    };
    template <typename C, typename Head>
    using get_overload_info_t = typename get_overload_info<C, Head>::type;

    template <bool, std::size_t i, typename C, typename... Args>
    struct invocable_traits_overload_info_impl
    {
        using type = get_overload_info_t<C, std::tuple_element_t<i, std::tuple<Args...>>>;
    };
    template <std::size_t i, typename C, typename... Args>
    struct invocable_traits_overload_info_impl<false, i, C, Args...>
    {
        static_assert(i < sizeof...(Args), "Argument index out of bounds (queried callable does not have this many matching overloads)");

        // to reduce excessive compiler error output
        using type = void;
    };

    template <typename C, bool B, typename... Args> struct invocable_traits_overload_info;
    template <typename C, bool B, typename... Args>
    struct invocable_traits_overload_info<C, B, typelist<Args...>>
    {
        static constexpr std::size_t num_matched_overloads  = sizeof...(Args);
        static constexpr auto        is_exact_match         = B;

        template <std::size_t i>
        using matched_overload = typename invocable_traits_overload_info_impl<
            i < sizeof...(Args),
            i,
            C,
            Args...
        >::type;
    };

    // found at least one overload taking a const/ref qualified version of the specified argument types
    // that is different from those provided by the library user
    template <typename C, typename List>
    struct invocable_traits_extract_try_harder :
        invocable_traits_impl<              // instantiate for the first matched overload
            std::decay_t<
                decltype(
                    invocable_traits_resolve_overload<C>(
                        &C::operator(),
                        typename try_harder::get_head<List>::type{}
                    )
                )
            >
        > ,
        invocable_traits_overload_info<     // but expose all matched overloads
            C,
            false,
            List
        >
    {};

    // failed to find any overload taking the specified argument types or some const/ref qualified version of them
    template <typename T>
    struct invocable_traits_extract_try_harder<T, typelist<>> : // empty list -> no combination of arguments matched an overload
        invocable_traits_error,
        invocable_traits_error_overload
    {
        static constexpr Error error = get_error<T, true>();
    };

    // specific overloaded operator() is available, use it for analysis
    template <typename C, bool, typename... OverloadArgs>
    struct invocable_traits_extract :
        invocable_traits_impl<
            std::decay_t<
                decltype(
                    invocable_traits_resolve_overload<C>(
                        &C::operator(),
                        typelist<OverloadArgs...>{}
                    )
                )
            >
        >,
        invocable_traits_overload_info<
            C,
            true,
            typelist<typelist<OverloadArgs...>> // expose matched overload through this interface also, for consistency, even though matching procedure was not run
        >
    {};

    // unambiguous operator() is available, use it for analysis
    template <typename C, bool B>
    struct invocable_traits_extract<C, B> :
        invocable_traits_impl<
            decltype(
                &C::operator()
            )
        > {};

    // no specific overloaded operator() taking the specified arguments is available, try harder
    // to see if one can be found that takes some other const/reference qualified version of the
    // input arguments.
    template <typename T, typename... OverloadArgs>
    struct invocable_traits_extract<T, false, OverloadArgs...> :
        invocable_traits_extract_try_harder<
            T,
            typename try_harder::filter<T,                          // filter list of all argument combinations: leave only resolvable overloads
                try_harder::list_product_t<                         // cartesian product of these lists
                    typename try_harder::type_maker_for_typelist<   // produce list with all const/ref combinations of each argument
                        OverloadArgs...
                    >::type
                >
            >::type
        > {};

    template <typename T>
    struct invocable_traits_extract<T, false> : invocable_traits_error
    {
        static constexpr Error error = get_error<T, false>();
    };

    // catch all that doesn't match the various function signatures above
    // If T has an operator(), we go with that. Else, issue error message.
    template <typename T>
    struct invocable_traits_impl<T> :
        invocable_traits_extract<
            T,
            HasCallOperator<T>&& CanGetCallOperator<T>
        > {};

    // if overload argument types are provided and needed, use them
    template <typename T, bool B, typename... OverloadArgs>
    struct invocable_traits_overload_impl :
        invocable_traits_extract<
            T, 
            HasCallOperator<T> && HasSpecificCallOperator<T, OverloadArgs...>,
            OverloadArgs...
        > {};

    // if they are provided but not needed, ignore them
    template <typename T, typename... OverloadArgs>
    struct invocable_traits_overload_impl<T, false, OverloadArgs...> :
        invocable_traits_impl<
            T
        > {};
}

template <typename T, typename... OverloadArgs>
struct get :
    detail::invocable_traits_overload_impl<
        std::remove_reference_t<T>,
        detail::HasCallOperator<T> && !detail::CanGetCallOperator<T>,
        OverloadArgs...
    > {};

template <typename T, typename... OverloadArgs>
struct get<std::reference_wrapper<T>, OverloadArgs...> :
    detail::invocable_traits_overload_impl<
        std::remove_reference_t<T>,
        detail::HasCallOperator<T> && !detail::CanGetCallOperator<T>,
        OverloadArgs...
    > {};

template <typename T>
struct get<T> :
    detail::invocable_traits_impl<
        std::decay_t<T>
    > {};

template <typename T>
struct get<std::reference_wrapper<T>> :
    detail::invocable_traits_impl<
        std::decay_t<T>
    > {};
}
