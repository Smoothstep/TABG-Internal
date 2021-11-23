constexpr auto RequestLoad = WM_USER + 444;
constexpr auto RequestUnhook = WM_USER + 445;

struct STargetAssembly
{
	char ModulePath[MAX_PATH];
	char Method[1024];
};