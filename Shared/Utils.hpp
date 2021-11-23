#pragma once

#include <concepts>
#include <span>
#include <optional>
#include <iostream>

template<class Fn>
class Finalizer
{
public:
	constexpr Finalizer(const Finalizer&) noexcept = delete;
	constexpr Finalizer(Finalizer&& Other) noexcept
		: Func{ std::move(Other.Func) }
	{
		Other.Func = {};
	}

	constexpr explicit Finalizer(Fn&& Func) noexcept
		: Func{ std::move(Func) }
	{}

	constexpr ~Finalizer()
	{
		if (Func)
		{
			std::invoke(*Func);
		}
	}

private:
	std::optional<Fn> Func;
};

template<class TFn>
struct FnTraits;

template<class TResult, class... TArgs>
struct FnTraits<TResult(*)(TArgs...)>
{
    using Result = TResult;

    constexpr FnTraits() noexcept = default;
    constexpr FnTraits(TResult(*)(TArgs...)) noexcept {}
};

template<class TFn>
using FnResult = typename FnTraits<TFn>::Result;

template<auto Create, auto Drop, auto InvalidHandle = nullptr>
    requires std::invocable<decltype(Drop), FnResult<decltype(Create)>>
class ScopedHandle
{
    using TCreate = decltype(Create);
    using THandle = typename FnResult<TCreate>;

public:
    constexpr ScopedHandle(const ScopedHandle&) = delete;
    constexpr ScopedHandle() noexcept = default;

    constexpr ScopedHandle(ScopedHandle&& Other)
        : Handle{ Other.Handle }
    {
        Other.Handle = InvalidHandle;
    }

    template<class... TArgs>
        requires std::invocable<decltype(Create), TArgs...>
    constexpr ScopedHandle(TArgs&&... Args) noexcept
        : Handle(std::invoke(Create, std::forward<TArgs>(Args)...))
    {}

    constexpr ~ScopedHandle() noexcept
    {
        if (*this)
        {
            std::invoke(Drop, Handle);
        }
    }

    constexpr auto operator*() -> THandle const
    {
        return Handle;
    }

    constexpr operator THandle() const
    {
        return Handle;
    }

    constexpr operator bool() const
    {
        return Handle != InvalidHandle;
    }

private:
    THandle Handle;
};

struct StringView : std::string_view
{
    using std::string_view::basic_string_view;

    template<class Rng>
    constexpr StringView(const Rng& Sv)
        requires requires(const Rng& R) { std::is_same_v<std::remove_const_t<decltype(*R.begin())>, char&>; }
    : std::string_view(&*Sv.begin(), std::ranges::distance(Sv))
    {}
};

template<class T>
concept Streamable = requires(T && F) { std::cout << F; };

template<Streamable... TArgs>
auto Log(TArgs&&... Args) -> std::ostream&
{
    return ((std::cout << "[INFO]   ") << ... << Args) << std::endl;
}

template<Streamable... TArgs>
auto Err(TArgs&&... Args) -> std::ostream&
{
    return ((std::cerr << "[ERROR]  ") << ... << Args) << std::endl;
}

constexpr auto operator"" _sp(const char* Data, size_t Len) -> std::span<const uint8_t>
{
    return { reinterpret_cast<const uint8_t*>(Data), Len };
}

auto LogInit() -> bool;