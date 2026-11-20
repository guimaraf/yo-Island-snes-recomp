/*
 * Super Mario World 2 - Yoshi's Island — desktop host shim.
 *
 * Identity and hooks, nothing else. The host itself is the framework's
 * (snesrecomp/runner/src/desktop/host_main.h): the pre-boot launcher, ROM
 * resolution and digest checks, config.ini and keybinds.ini, the window and
 * the SDL / OpenGL presenters, audio, gamepads, the in-game save-state
 * browser and rewind filmstrip, the OSD, the pacing clock, crash handlers and
 * the post-mortem report, mod packages, Generate & rebuild, and the netplay
 * barrier when the project is built with it. A fix there reaches this
 * project on a submodule pull; nothing in this file needs to change for it.
 *
 * This file used to BE the host -- a ~700-line copy made at scaffold time --
 * so every project scaffolded before a fix kept the bug. Two ports of the
 * same game, a week apart, had different hosts.
 *
 * Nothing here ever ships a ROM. The player supplies one; the host makes
 * that easy (launcher, then positional argument, then a copy beside the
 * executable, then the rom.cfg cache, then a file picker) and checks what it
 * is handed against the digests in rom_identity.txt.
 *
 * Grow this file only with what is specific to THIS title: a custom
 * presenter (prepare_frame / draw_frame), an SPC player, a pacing rule
 * (keep_pacing_debt), a Mods provider. See SnesDesktopHostGame for the
 * complete list of hooks and what each is for.
 */

#if defined(_WIN32) && defined(SNESRECOMP_SETUP_HOST)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* The runner defines its own FORCEINLINE for its C headers. */
#undef FORCEINLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sha256.h"
#endif

#include "host_main.h"
#include "recomp_launcher.h"
#include "game_rtl.h"
#include "snesrecomp_rom_identity.h"  /* generated from rom_identity.txt */
#ifndef __ANDROID__
#include "snesrecomp_codegen_host.h"
#endif

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED 1
#endif
#include "desktop/sdl_compat.h"
#ifdef __ANDROID__
/* SDLActivity loads libmain.so and dlsyms its SDL_main. */
#define main SDL_main
#endif

#ifndef SNES_GAME_VERSION
#define SNES_GAME_VERSION "dev"
#endif

static void YoshiConfigureLauncher(struct RecompLauncherCGameInfo *info)
{
    if (!info) return;
    info->has_run_ahead = 0;
#ifndef __ANDROID__
    if (!info->in_session) {
        static const SnesrecompCodegenHostConfig config = {
            .display_name = "Super Mario World 2 - Yoshi's Island",
            .cmake_target = "SuperMarioWorld2YoshiSIslandSNESRecomp",
            .exe_basename = YI_CODEGEN_EXE_NAME,
            .snesrecomp_cli_relpath = YI_CODEGEN_CLI,
            .expected_crc32 = SNESRECOMP_ROM_EXPECTED_CRC32,
            .expected_sha256 = SNESRECOMP_ROM_EXPECTED_SHA256,
            .cfg_roots = 1,
        };
        snesrecomp_codegen_host_apply(info, &config);
    }
#endif
}

static const SnesDesktopHostGame kGameHost = {
    .display_name        = "Super Mario World 2 - Yoshi's Island",
    .window_title        = "Super Mario World 2 - Yoshi's Island",
    .region              = SNESRECOMP_ROM_REGION,
    .rom_file            = SNESRECOMP_ROM_FILE,
    .expected_sha256_hex = SNESRECOMP_ROM_EXPECTED_SHA256,
    .expected_crc32_hex  = SNESRECOMP_ROM_EXPECTED_CRC32,
    .game_id             = SNESRECOMP_ROM_GAME_ID,
    .build_version       = SNES_GAME_VERSION,
    .game_info           = &kGameInfo,
    .configure_launcher  = YoshiConfigureLauncher,
    .num_players         = 2,
    /* Battery-backed SRAM shows the launcher's SAVES panel. Leave NULL for a
     * title without one. The path is exe-relative. */
    .sram_path           = "saves/save.srm",
};

#ifndef __ANDROID__
#undef main   /* desktop: keep plain main() even if SDL_main.h remapped it */
#endif

#if defined(_WIN32) && defined(SNESRECOMP_SETUP_HOST)
/* RecompLauncher invokes these modes on the unpacked setup host. Only the
 * executable under build/ is replaced, so this build can run synchronously. */
#define EASY_GAME_EXE "build\\" YI_CODEGEN_EXE_NAME ".exe"
#define EASY_READY "build\\recomplauncher-ready-" SNES_GAME_VERSION
#define EASY_LOG "build\\recomplauncher-build.log"

static int EasyFileExists(const char *path)
{
    const DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static int EasyRomMatches(const char *path)
{
    /* The framework's interactive verifier shows a message box on mismatch.
     * Keep the same headerless digest check with bounded, headless IO. */
    enum { size = 2 * 1024 * 1024, header = 512 };
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    long length = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    uint8_t *data = NULL;
    if ((length == size || length == size + header) &&
        fseek(file, length - size, SEEK_SET) == 0)
        data = malloc(size);
    int matches = 0;
    if (data && fread(data, 1, size, file) == size) {
        uint8_t digest[32];
        char hex[65];
        sha256_compute(data, size, digest);
        for (size_t i = 0; i < sizeof(digest); ++i)
            snprintf(hex + i * 2, 3, "%02x", (unsigned)digest[i]);
        matches = strcmp(hex, SNESRECOMP_ROM_EXPECTED_SHA256) == 0;
    }
    free(data);
    fclose(file);
    return matches;
}

static void EasyProgress(void *context, float fraction, const char *message)
{
    const int stage = *(const int *)context;
    const unsigned step = (unsigned)(100 * fraction);
    const unsigned percent = stage == 0 ? step * 40 / 100 : 40 + step * 25 / 100;
    printf("progress 0.%02u %s\n", percent, message);
    fflush(stdout);
}

static DWORD EasyRun(const char *program, char *command, HANDLE log)
{
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION child;
    startup.cb = sizeof(startup);
    if (log != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = NULL;
        startup.hStdOutput = log;
        startup.hStdError = log;
    }
    if (!CreateProcessA(program, command, NULL, NULL, log != INVALID_HANDLE_VALUE,
                        CREATE_NO_WINDOW, NULL, NULL, &startup, &child)) {
        fprintf(stderr, "Could not start %s (Windows error %lu).\n", program,
                (unsigned long)GetLastError());
        return 1;
    }
    CloseHandle(child.hThread);
    DWORD result = 1;
    if (WaitForSingleObject(child.hProcess, INFINITE) == WAIT_OBJECT_0)
        GetExitCodeProcess(child.hProcess, &result);
    CloseHandle(child.hProcess);
    return result;
}

static int EasyBuild(void)
{
    char program[MAX_PATH] = "cmake.exe";
    char command[2 * MAX_PATH + 300];
    const DWORD length = GetEnvironmentVariableA("RETCOMM_TOOLCHAIN_DIR",
                                                 program, sizeof(program));
    if (length && length <= sizeof(program) - sizeof("\\bin\\cmake.exe"))
        memcpy(program + length, "\\bin\\cmake.exe", sizeof("\\bin\\cmake.exe"));
    else
        program[0] = '\0';
    if (!EasyFileExists(program)) {
        DWORD found = SearchPathA(NULL, "cmake.exe", NULL, sizeof(program), program, NULL);
        if (!found || found >= sizeof(program))
            return 0;
    }

    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), NULL, TRUE};
    HANDLE log = CreateFileA(EASY_LOG, GENERIC_WRITE, FILE_SHARE_READ, &attributes,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE)
        return 0;

    puts("progress 0.66 Configuring Release build");
    fflush(stdout);
    snprintf(command, sizeof(command),
             "\"%s\" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "
             "-DSNESRECOMP_SETUP_HOST=OFF", program);
    DWORD result = EasyRun(program, command, log);
    if (!result) {
        puts("progress 0.72 Compiling the game");
        fflush(stdout);
        snprintf(command, sizeof(command),
                 "\"%s\" --build build --parallel --target "
                 "SuperMarioWorld2YoshiSIslandSNESRecomp", program);
        result = EasyRun(program, command, log);
    }
    CloseHandle(log);
    return !result && EasyFileExists(EASY_GAME_EXE);
}

static int EasyPrepare(const char *rom)
{
    if (!CreateDirectoryA("build", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        fputs("Could not create the build directory.\n", stderr);
        return 1;
    }
    HANDLE lock = CreateFileA("build\\recomplauncher-setup.lock", GENERIC_WRITE,
                              0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE) {
        fputs("Could not lock the build directory for preparation.\n", stderr);
        return 1;
    }

    char error[512] = "Could not initialize the setup tools.";
    char prepared_path[MAX_PATH];
    int stage = 0;
    int result = 1;
    RecompLauncherCGameInfo info = {0};
    /* Applying the shared callbacks may hand off to an existing build. */
    _putenv_s("SNESRECOMP_NO_HANDOFF", "1");
    _putenv_s("SNESRECOMP_BUILD_DIR", "build");
    YoshiConfigureLauncher(&info);
    if (!DeleteFileA(EASY_READY) && GetLastError() != ERROR_FILE_NOT_FOUND) {
        snprintf(error, sizeof(error), "Could not clear the previous setup marker.");
    } else if (info.ensure_toolchain_with_progress && info.prepare_with_progress &&
               info.ensure_toolchain_with_progress(1, NULL, error, sizeof(error),
                                                   EasyProgress, &stage)) {
        stage = 1;
        if (info.prepare_with_progress(rom, prepared_path, sizeof(prepared_path),
                                       error, sizeof(error), EasyProgress, &stage)) {
            snprintf(error, sizeof(error), "Build failed. See %s.", EASY_LOG);
            if (EasyBuild()) {
                HANDLE ready = CreateFileA(EASY_READY, GENERIC_WRITE, 0, NULL,
                                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (ready != INVALID_HANDLE_VALUE) {
                    CloseHandle(ready);
                    puts("progress 1 Setup complete");
                    result = 0;
                } else {
                    snprintf(error, sizeof(error), "Could not write the setup marker.");
                }
            }
        }
    }
    if (result)
        fprintf(stderr, "%s\n", error);
    CloseHandle(lock);
    return result;
}

static int EasySetupCommand(int argc, char **argv)
{
    const int prepare = strcmp(argv[1], "--easy-prepare") == 0;
    char rom[MAX_PATH];
    if (argc != 3) {
        fprintf(stderr, "Usage: %s %s <ROM path>\n", argv[0], argv[1]);
        return 2;
    }
    const DWORD rom_length = GetFullPathNameA(argv[2], sizeof(rom), rom, NULL);
    if (!rom_length || rom_length >= sizeof(rom) || !EasyFileExists(rom)) {
        fputs("The selected ROM file is missing or its path is unsupported.\n", stderr);
        return 2;
    }
    char root[MAX_PATH];
    const DWORD root_length = GetModuleFileNameA(NULL, root, sizeof(root));
    if (!root_length || root_length >= sizeof(root))
        return 2;
    char *separator = strrchr(root, '\\');
    if (!separator)
        return 2;
    separator[1] = '\0';
    if (!SetCurrentDirectoryA(root))
        return 2;
    if (!EasyRomMatches(rom)) {
        fputs("Select the original USA Yoshi's Island ROM with the expected SHA-256.\n", stderr);
        return 1;
    }
    if (prepare)
        return EasyPrepare(rom);
    if (!EasyFileExists(EASY_READY) || !EasyFileExists(EASY_GAME_EXE)) {
        fputs("Run Easy Setup before starting the game.\n", stderr);
        return 1;
    }
    char command[MAX_PATH + 100];
    snprintf(command, sizeof(command), "\"%s\" --no-launcher --rom \"%s\"",
             EASY_GAME_EXE, rom);
    return (int)EasyRun(EASY_GAME_EXE, command, INVALID_HANDLE_VALUE);
}
#endif

int main(int argc, char **argv)
{
#if defined(_WIN32) && defined(SNESRECOMP_SETUP_HOST)
    if (argc > 1 && (!strcmp(argv[1], "--easy-prepare") ||
                     !strcmp(argv[1], "--easy-play")))
        return EasySetupCommand(argc, argv);
#endif
    return snesrecomp_desktop_main(&kGameHost, argc, argv);
}
