#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>

namespace {
std::wstring quoteArgument(const std::wstring &argument) {
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;

  std::wstring quoted(1, L'"');
  std::size_t backslashes = 0;
  for (const wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    if (character == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(L'"');
    } else {
      quoted.append(backslashes, L'\\');
      quoted.push_back(character);
    }
    backslashes = 0;
  }
  quoted.append(backslashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

int showLaunchError(const wchar_t *message) {
  MessageBoxW(nullptr, message, L"AniCloud", MB_OK | MB_ICONERROR);
  return EXIT_FAILURE;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  std::wstring launcherPath(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, launcherPath.data(), static_cast<DWORD>(launcherPath.size()));
  if (length == 0 || length == launcherPath.size()) return showLaunchError(L"AniCloud could not locate its installation directory.");
  launcherPath.resize(length);

  const std::size_t separator = launcherPath.find_last_of(L"\\/");
  if (separator == std::wstring::npos) return showLaunchError(L"AniCloud could not locate its installation directory.");
  const std::wstring installDirectory = launcherPath.substr(0, separator);
  const std::wstring applicationPath = installDirectory + L"\\bin\\AniCloud.exe";
  if (GetFileAttributesW(applicationPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    return showLaunchError(L"AniCloud is incomplete. Reinstall the application to restore bin\\AniCloud.exe.");

  int argumentCount = 0;
  LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
  if (!arguments) return showLaunchError(L"AniCloud could not read its launch arguments.");

  std::wstring commandLine = quoteArgument(applicationPath);
  for (int index = 1; index < argumentCount; ++index) {
    commandLine.push_back(L' ');
    commandLine += quoteArgument(arguments[index]);
  }
  LocalFree(arguments);

  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION processInfo{};
  if (!CreateProcessW(applicationPath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      installDirectory.c_str(), &startupInfo, &processInfo))
    return showLaunchError(L"AniCloud could not start. Reinstall the application and try again.");

  WaitForSingleObject(processInfo.hProcess, INFINITE);
  DWORD exitCode = EXIT_FAILURE;
  GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hThread);
  CloseHandle(processInfo.hProcess);
  return static_cast<int>(exitCode);
}
