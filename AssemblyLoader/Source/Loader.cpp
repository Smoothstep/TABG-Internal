#include <algorithm>
#include <map>
#include <set>
#include <ranges>
#include <array>
#include <charconv>
#include <fstream>
#include <format>
#include <filesystem>

#include "Loader.hpp"

#include <psapi.h>

using namespace std::string_view_literals;

class TempFilePathCopy : public std::filesystem::path
{
	TempFilePathCopy() noexcept = default;
	TempFilePathCopy(std::filesystem::path&& Path, bool Delete) noexcept
		: std::filesystem::path{ std::move(Path) }
		, Delete{ Delete }
	{}

public:
	TempFilePathCopy(const TempFilePathCopy&) = delete;
	TempFilePathCopy(TempFilePathCopy&& Other) noexcept
		: std::filesystem::path{ std::move(Other) }
		, Delete{ Other.Delete }
	{
		Other.Delete = false;
	}

	static auto Create(const std::filesystem::path& FromAbs, std::string_view ToRel) 
		-> std::tuple<std::optional<TempFilePathCopy>, std::error_code>
	{
		if (!FromAbs.has_filename())
		{
			return {};
		}

		auto NewPath = std::filesystem::current_path().append(ToRel).append(FromAbs.filename().string());

		std::error_code Ec;
		if (!std::filesystem::copy_file(FromAbs, NewPath, std::filesystem::copy_options::overwrite_existing, Ec))
		{
			return { TempFilePathCopy{}, Ec };
		}

		return { TempFilePathCopy{ std::move(NewPath), FromAbs != NewPath }, std::error_code{} };
	}

	~TempFilePathCopy()
	{
		if (Delete && std::filesystem::exists(*this))
		{
			std::filesystem::remove(*this);
		}
	}

private:
	bool Delete = false;
};

constexpr LoaderException::LoaderException(const std::string_view Err)
	: Error{ Err }
{}

LoaderException::LoaderException(const std::exception & Ex)
	: Error{ Ex.what() }
{}

auto EQU8AssemblyLoader::Create(std::string_view MonoModulePath, std::ostream& Out) -> std::optional<EQU8AssemblyLoader>
{
	HMODULE MonoModule = GetModuleHandle("mono-2.0-bdwgc.dll");

	if (!MonoModule)
	{
		Out << "No corresponding mono module found or loaded" << std::endl;
		return {};
	}
	else
	{
		EQU8AssemblyLoader Loader{ MonoModule, MonoModulePath };

		Out << "Disabling memory checks..." << std::endl;
		Loader.DisableRetAddrChecks();

		Out << "Unlinking hooks..." << std::endl;
		Loader.UnlinkHooks();

		Out << "Disabling assembly checker..." << std::endl;
		Loader.NopAssemblyChecker();

		Out << "Restoring symbols..." << std::endl;
		Loader.RestoreSymbols();

		Out << "Stealth patched" << std::endl;

		return { Loader };
	}
}

constexpr EQU8AssemblyLoader::EQU8AssemblyLoader(HMODULE Module, std::string_view MonoModulePath) noexcept
	: MonoModule{ Module }
	, MonoModulePath{ MonoModulePath }
{}

auto EQU8AssemblyLoader::LoadMonoModuleBytes() const -> std::vector<std::byte>
{
	std::vector<std::byte> Bytes;
	{
		std::ifstream Dll(MonoModulePath.data(), std::ios::in | std::ios::binary);
		if (!Dll.good())
		{
			throw LoaderException{ std::format("Failed to read mono module: {}", MonoModulePath) };
		}

		auto* Buf = Dll.rdbuf();
		Dll.seekg(0, Dll.end);
		
		const auto Size = Dll.tellg();
		if (Size <= 0)
		{
			throw LoaderException{ std::format("Failed to read mono module: {}", MonoModulePath) };
		}
		
		Bytes.resize(Dll.tellg());
		Dll.seekg(0);
		Buf->pubseekoff(0, Dll.beg);
		Buf->sgetn(reinterpret_cast<char*>(Bytes.data()), Bytes.size());
	}

	return Bytes;
}

auto EQU8AssemblyLoader::Load(
	const char* Path,
	const char* MethodDesc) const -> bool
{
	const HMODULE Module = MonoModule;

#define DECL_FN(Result, Name, Args) typedef Result(*p_##Name) Args;
#include "MonoFunctions.hpp"
#define DECL_FN(Result, Name, Args) p_##Name Name = reinterpret_cast<Result(*)Args>(GetProcAddress(Module, #Name)); if (!#Name) return false;
#include "MonoFunctions.hpp"

	const auto [LocalPath, Err] = TempFilePathCopy::Create(Path, ManagedPath);

	if (!LocalPath)
	{
		throw LoaderException{ std::format("Failed to create temp file. Ec: {}", Err.message()) };
	}

	MonoDomain* RootDomain = mono_get_root_domain();
	if (!RootDomain)
	{
		throw LoaderException{ "mono_get_root_domain returned null" };
	}

	MonoAssembly* Assembly = mono_domain_assembly_open(RootDomain, LocalPath->string().c_str());
	if (!Assembly)
	{
		throw LoaderException{ "mono_domain_assembly_open returned null" };
	}

	MonoImage* Image = mono_assembly_get_image(Assembly);
	if (!Image)
	{
		throw LoaderException{ "mono_assembly_get_image returned null" };
	}

	MonoMethodDesc* Desc = mono_method_desc_new(MethodDesc, true);
	if (!Desc)
	{
		throw LoaderException{ "mono_method_desc_new returned null" };
	}

	const Finalizer FreeDesc
	{
		[mono_method_desc_free, Desc]()
		{
			mono_method_desc_free(Desc);
		}
	};

	MonoMethod* Method = mono_method_desc_search_in_image(Desc, Image);
	if (!Method)
	{
		throw LoaderException{ "mono_method_desc_search_in_image returned null" };
	}

	mono_runtime_invoke(Method, 0, 0, 0);

	return true;
}

void EQU8AssemblyLoader::DisableRetAddrChecks() const
{
	for (DWORD N = 0x60; N < 0x70; ++N)
	{
		if (!TlsSetValue(N, reinterpret_cast<LPVOID>(1)))
		{
			throw LoaderException{ std::format("Failed to set tls value: {}", N) };
		}
	}
}

void EQU8AssemblyLoader::NopAssemblyChecker() const
{
	constexpr int NumNops = 3;
	constexpr int Nop = 0x90;

	auto Client = GetModuleHandleA("client.x64.equ8.dll");
	if (!Client)
	{
		throw LoaderException{ "Failed to obtain handle for client.x64.equ8.dll" };
	}

	MODULEINFO Info = {};
	if (!GetModuleInformation(GetCurrentProcess(), Client, &Info, sizeof(Info)))
	{
		throw LoaderException{ "Failed to obtain module information for client.x64.equ8.dll" };
	}

	constexpr auto CheckPattern = [](const auto Lhs, const auto Rhs)
	{ 
		return Lhs == Rhs || Rhs == '?';
	};

	const auto ModSpan = std::span{ reinterpret_cast<uint8_t*>(Info.lpBaseOfDll), Info.SizeOfImage };
	const auto Pattern = "\x48\x8D?????\x49\x8B?????\x41\xFF\xD4"_sp;

	auto Rng = std::ranges::search(ModSpan, Pattern, CheckPattern);
	if (!Rng)
	{
		const auto PatternPatch = "\x48\x8D?????\x49\x8B?????\x90\x90\x90"_sp;
		
		if (std::ranges::search(ModSpan, PatternPatch, CheckPattern))
		{
			return;
		}

		throw LoaderException{ std::format("Failed to find pattern {}", reinterpret_cast<const char*>(Pattern.data())) };
	}

	Rng = Rng | std::views::drop(Pattern.size() - NumNops);

	DWORD Old;
	if (!VirtualProtect(Rng.data(), Rng.size(), PAGE_EXECUTE_READWRITE, &Old))
	{
		throw LoaderException{ std::format("Failed to protect {}", reinterpret_cast<uintptr_t>(Rng.data())) };
	}

	std::fill(Rng.begin(), Rng.end(), Nop);

	if (!VirtualProtect(Rng.data(), Rng.size(), Old, &Old))
	{
		throw LoaderException{ std::format("Failed to restore protection {}", reinterpret_cast<uintptr_t>(Rng.data())) };
	}
}

void EQU8AssemblyLoader::UnlinkHooks() const
{
	constexpr size_t LoadHookOffset = 0x4998F0 + 0x400;
	constexpr size_t PreLoadHookOffset = 0x4999A8 + 0x400;

	const size_t BaseVa = reinterpret_cast<size_t>(MonoModule);

	AssemblyLoadHook** LoadHook = reinterpret_cast<AssemblyLoadHook**>(BaseVa + LoadHookOffset); // 0x7FFF9838A8F0
	AssemblyLoadHook** PreLoadHook = reinterpret_cast<AssemblyLoadHook**>(BaseVa + PreLoadHookOffset); // 0x7FFF9838A9A8

	if (*LoadHook)
	{
		(*LoadHook)->next = nullptr;
	}

	if (PreLoadHook)
	{
		*PreLoadHook = nullptr;
	}
}

void EQU8AssemblyLoader::RestoreSymbols() const
{
	std::array Affected =
	{
#include "MonoExportsAffected.inl"
	};

	RestoreSymbolsBc(Affected, LoadMonoModuleBytes());
}

template<
	std::ranges::range RngSymbol,
	std::ranges::range RngModule
>
requires requires(RngSymbol S, RngModule M)
{
	{ *std::begin(S) } -> std::convertible_to<std::string_view>;
	{ *std::begin(M) } -> std::convertible_to<std::byte>;
}
void EQU8AssemblyLoader::RestoreSymbolsBc(RngSymbol Syms, const RngModule& MonoModuleBytes) const
{
	constexpr size_t PatchByteCount = 16;

	for (auto Sym : Syms)
	{
		auto Proc = GetProcAddress(MonoModule, std::string_view{ Sym }.data());
		if (Proc)
		{
			const ptrdiff_t Offset = 
				reinterpret_cast<ptrdiff_t>(Proc) -
				reinterpret_cast<ptrdiff_t>(MonoModule) - 0xC00;

			if (Offset + PatchByteCount > MonoModuleBytes.size())
			{
				throw LoaderException{ std::format("Symbol out of range: {}", Sym) };
			}

			if (memcmp(Proc, &MonoModuleBytes[Offset], PatchByteCount) != 0)
			{
				DWORD Old;

				if (!VirtualProtect(Proc, PatchByteCount, PAGE_EXECUTE_READWRITE, &Old))
				{
					throw LoaderException{ "VirtualProtect apply PAGE_EXECUTE_READWRITE failed" };
				}

				memcpy(Proc, &MonoModuleBytes[Offset], PatchByteCount);

				if (!VirtualProtect(Proc, PatchByteCount, Old, &Old))
				{
					throw LoaderException{ "VirtualProtect restore failed" };
				}
			}
		}
		else
		{
			throw LoaderException{ std::format("Symbol not found: {}", Sym) };
		}
	}
}

/*
void EQU8AssemblyLoader::RestoreSymbols(std::optional<std::string_view> SymbolsFile) const
{
	if (SymbolsFile)
	{
		const std::ifstream SymFile(SymbolsFile->data(), std::ios::in);

		if (!SymFile.good())
		{
			throw LoaderException{ "Failed to open symbol file: Symbols.txt" };
		}

		std::stringstream Ss;
		Ss << SymFile.rdbuf();
		{
			const std::set<std::string_view> AffectedFunctions =
			{
#include "MonoExportsAffected.hx"
			};

			auto Syms = std::string_view{ Ss.str() }
				| std::views::split('\n')
				| std::views::transform([](StringView Sv)
			{
				const auto SmOffset = Sv | std::views::split(' ');

				const StringView Sym{ *SmOffset.begin() };
				const StringView Off{ *SmOffset.begin()++ };

				size_t Offset = 0;

				if (auto Ec = std::from_chars(&Off.front(), &Off.back(), Offset); Ec.ec != std::errc{})
				{
					throw LoaderException{ std::format("Symbols file format invalid: Failed to parse offset for: {}", Sv) };
				}

				return std::tuple<size_t, std::string_view>{ Offset, Sym };
			}) | std::views::filter([&AffectedFunctions](const auto& Tpl)
			{
				return AffectedFunctions.find(std::get<1>(Tpl)) != AffectedFunctions.end();
			});

			RestoreSymbolsBc(Syms, LoadMonoModuleBytes());
		}
	}
	else
	{
		constexpr std::array Syms =
		{
#include "MonoExports.hx"
		};

		constexpr auto Filtered = Syms | std::views::filter([](const auto& Tpl) -> bool
		{
			constexpr std::array Affected =
			{
		#include "MonoExportsAffected.hx"
			};

			return std::ranges::includes(Affected, std::ranges::single_view{ std::get<1>(Tpl) }, [](auto Lhs, auto Rhs)
			{
				return Lhs.compare(Rhs) == 0;
			});
		});

		RestoreSymbolsBc(Filtered, LoadMonoModuleBytes());
	}
}
*/

