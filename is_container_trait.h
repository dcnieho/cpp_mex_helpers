#pragma once

template <class ContainerType>
concept Container = requires(ContainerType a)
{
    requires std::regular<ContainerType>;
    requires std::swappable<ContainerType>;
    requires std::destructible<typename ContainerType::value_type>;
    requires std::same_as<typename ContainerType::reference, typename ContainerType::value_type&>;
    requires std::same_as<typename ContainerType::const_reference, const typename ContainerType::value_type&>;
    requires std::forward_iterator<typename ContainerType::iterator>;
    requires std::forward_iterator<typename ContainerType::const_iterator>;
    requires std::unsigned_integral<typename ContainerType::size_type>;
    requires std::signed_integral<typename ContainerType::difference_type>;
    requires std::same_as<typename ContainerType::difference_type, typename std::iterator_traits<typename ContainerType::iterator>::difference_type>;
    requires std::same_as<typename ContainerType::difference_type, typename std::iterator_traits<typename ContainerType::const_iterator>::difference_type>;
    { a.begin() } -> std::convertible_to<typename ContainerType::iterator>;
    { a.end() } -> std::convertible_to<typename ContainerType::iterator>;
    { a.cbegin() } -> std::convertible_to<typename ContainerType::const_iterator>;
    { a.cend() } -> std::convertible_to<typename ContainerType::const_iterator>;
    { a.size() } -> std::convertible_to<typename ContainerType::size_type>;
    { a.max_size() } -> std::convertible_to<typename ContainerType::size_type>;
    { a.empty() } -> std::convertible_to<bool>;
};

template <class ContainerType>
concept ContiguousStorage = std::contiguous_iterator<typename ContainerType::const_iterator>;