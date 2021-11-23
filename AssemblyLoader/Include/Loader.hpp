#pragma once

#include <Windows.h>

#include <ostream>
#include <optional>

#include <vector>
#include <type_traits>

#include <string>
#include <string_view>

#include "Utils.hpp"
#include "MonoTypes.hpp"
#include "Messages.hpp"

constexpr std::string_view ManagedPath = "TotallyAccurateBattlegrounds_Data/Managed/";

class LoaderException
{
public:
	constexpr LoaderException() = default;
	constexpr LoaderException(const LoaderException&) = default;
	constexpr LoaderException(LoaderException&&) = default;
	constexpr explicit LoaderException(const std::string_view Err);
	LoaderException(const std::exception& Ex);

	constexpr auto What() const -> std::string_view;

private:
	std::string Error;
};

constexpr auto LoaderException::What() const -> std::string_view
{
	return Error;
}

class EQU8AssemblyLoader
{
public:
	static auto Create(std::string_view MonoModulePath, std::ostream& Out)
		->std::optional<EQU8AssemblyLoader>;

	auto Load(
		const char* Path,
		const char* MethodDesc) const -> bool;

private:
	constexpr EQU8AssemblyLoader(HMODULE Module, std::string_view MonoModulePath) noexcept;

	auto LoadMonoModuleBytes() const -> std::vector<std::byte>;
	void DisableRetAddrChecks() const;
	void NopAssemblyChecker() const;
	void UnlinkHooks() const;
	void RestoreSymbols() const;

	template<
		std::ranges::range RngSymbol,
		std::ranges::range RngModule
	>
	requires requires(RngSymbol S, RngModule M)
	{
		{ *std::begin(S) } -> std::convertible_to<std::string_view>;
		{ *std::begin(M) } -> std::convertible_to<std::byte>;
	}
	void RestoreSymbolsBc(RngSymbol Syms, const RngModule& MonoModuleBytes) const;

	HMODULE MonoModule;
	std::string MonoModulePath;
};